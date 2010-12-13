## @file
## This script implements quest for two-hand mastery, given by Mercenary
## Taleus in the Brynknot Mercenary Guild.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

## Info about the quest.
quest = {
	"quest_name": "Dark Cave Elder Wyverns",
	"type": 2,
	"arch_name": "tooth",
	"item_name": "elder wyvern tooth",
	"message": "Kill elder wyvern in a cave Southeast of Brynknot and bring back his tooth.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

## Get the activator's physical experience object, so we can check if
## they are high enough level for the quest.
eobj = activator.Controller().GetSkill(Type.EXPERIENCE, EXP_PHYSICAL)

msg = WhatIsMessage().strip().lower()
text = msg.split()

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if qm.started() and qm.completed():
			me.SayTo(activator, "\nHello {0}.\nGood to see you back.".format(activator.name))
			return

		if not qm.finished():
			me.SayTo(activator, "\nHello {0}.\nI am the current ^archery^ commander after ^Chereth^ lost her eyes in this terrible fight with the ^wyverns^.".format(activator.name))
		else:
			me.SayTo(activator, "\nAh, you are back.\nAnd I see you have the tooth!\nNow I will ^teach^ you two-hand mastery!")

	elif text[0] == "archery" or text[0] == "chereth":
		me.SayTo(activator, "\nYou should ask Chereth about the three archery skills.\nShe still teaches archery and her knowledge about it is superior.\nAfter she lost her eyes she was transferred to the Tutorial Island.")

	elif text[0] == "teach":
		if qm.started() and qm.completed():
			me.SayTo(activator, "\nI can't teach you more.")
			return

		## Get the activator's physical experience object, so we can check if
		## they are high enough level for the quest.
		eobj = activator.Controller().GetSkill(Type.EXPERIENCE, EXP_PHYSICAL)

		if not qm.finished():
			me.SayTo(activator, "\nFirst bring me the elder wyvern tooth!")
			return
		elif not eobj or eobj.level < 10:
			me.SayTo(activator, "\nYour level is too low. Come back later!")
			return

		skill = GetSkillNr("two-hand mastery")
		sobj = activator.Controller().GetSkill(Type.SKILL, skill)

		qm.complete()
		activator.Write("Taleus takes {0} from your inventory.".format(quest["item_name"]), COLOR_WHITE)

		if sobj != None:
			me.SayTo(activator, "\nYou already know that skill?!")
		else:
			me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Taleus teaches some ancient skill.", COLOR_YELLOW)
			activator.Controller().AcquireSkill(skill)

	elif text[0] == "two-hand":
		me.SayTo(activator, "\nTwo-hand mastery will allow you to fight with two-hand weapons. You will do more damage and hit better at the cost of lower protection because you can't wield a shield.")

	elif text[0] == "accept":
		if not qm.started():
			me.SayTo(activator, "\nReturn to me with the elder wyvern tooth and I will reward you.")
			qm.start()

	elif text[0] == "elder":
		if qm.started() and qm.completed():
			return

		if not qm.finished():
			me.SayTo(activator, "\nThe elder wyverns are the most aggressive and strongest of the wyverns in that cave.\nIf you can kill one or two I will help you too.\nI'll make a deal with you:\nBring me the tooth of an elder wyvern and I will teach you ^two-hand^ mastery.")

			if not qm.started():
				me.SayTo(activator, "Do you ^accept^ this quest?", 1)

	elif text[0] == "wyverns" or text[0] == "wyvern":
		if qm.started() and qm.completed():
			return

		if not qm.finished():
 			me.SayTo(activator, "\nThe wyverns live in a big cave southeast of Brynknot.\nThey are dangerous and attacked us several times.\nWe have sent some expeditions but there are a lot of them.\nThe biggest problem are the ^elder^ wyverns.")

main()
