## @file
## This script implements quest for polearm mastery, given by Mercenary
## Lepash in the Brynknot Mercenary Guild.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

## Info about the quest.
quest = {
	"quest_name": "Hill Giants Stronghold",
	"type": 2,
	"arch_name": "horn",
	"item_name": "clan horn of the hill giants",
	"message": "Kill the leader giant of the Hill Giants Stronghold North of Brynknot and bring back his horn.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

msg = WhatIsMessage().strip().lower()
text = msg.split()

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if qm.started() and qm.completed():
			me.SayTo(activator, "\nHello {0}.\nGood to see you back.".format(activator.name))
			return

		if not qm.finished():
			me.SayTo(activator, "\nHello there. I am guard commander Lepash.\nI have taken ^home^ here in this mercenary guild.\nHmm, are you interested in a ^job^?")
		else:
			me.SayTo(activator, "\nAh, you are back.\nAnd I see you have the clan horn!\nNow I will ^teach^ you polearm mastery.")

	elif text[0] == "home":
		me.SayTo(activator, "\nYes, we still have heavy logistic problems.\nWe don't have half the men we need to control half of the area we should.\nWe don't have enough supply and enough rooms.\nWell, it means all is normal if you do such a heavy invasion like we're doing at the moment.")

	elif text[0] == "teach":
		if qm.started() and qm.completed():
			me.SayTo(activator, "\nI can't teach you more.")
			return

		## Get the activator's physical experience object, so we can check if
		## they are high enough level for the quest.
		eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_PHYSICAL)

		if not qm.finished():
			me.SayTo(activator, "\nFirst bring me the clan horn of the hill giants!")
			return
		elif not eobj or eobj.level < 11:
			me.SayTo(activator, "\nYour level is too low. Come back later!")
			return

		skill = GetSkillNr("polearm mastery")
		sobj = activator.GetSkill(TYPE_SKILL, skill)

		qm.complete()
		activator.Write("Lepash takes {0} from your inventory.".format(quest["item_name"]), COLOR_WHITE)

		if sobj != None:
			me.SayTo(activator, "\nYou already know that skill?!")
		else:
			me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Lepash teaches some ancient skill.", COLOR_YELLOW)
			activator.AcquireSkill(skill)

	elif text[0] == "polearm" or text[0] == "polearms":
		me.SayTo(activator, "\nPolearm mastery will allow you to fight with polearm weapons. You will do a lot more damage and you will have some protection even though you can't wear a shield using polearms.")

	elif text[0] == "accept":
		if not qm.started():
			me.SayTo(activator, "\nReturn to me with the clan horn and I will reward you.")
			qm.start()

	elif text[0] == "job":
		if qm.started() and qm.completed():
			return

		if not qm.finished():
			me.SayTo(activator, "\nThere is somewhere north of Brynknot a hill giant camp.\nPerhaps in a cave or something.\nWe noticed them around here.\nFind this hill giant camp and kill the camp leader.\nHe should have a sign of power like a clan horn or something. Show it to me and I will teach you ^polearm^ mastery.")

			if not qm.started():
				me.SayTo(activator, "Do you ^accept^ this quest?", 1)

main()
