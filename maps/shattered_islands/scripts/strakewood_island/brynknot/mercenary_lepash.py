## @file
## This script implements quest for polearm mastery, given by Mercenary
## Lepash in the Brynknot Mercenary Guild.

from Atrinik import *
import string
from QuestManager import QuestManager

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Info about the quest for QuestManager.
quest = {
	"quest_name": "Hill Giants Stronghold",
	"type": 2,
	"arch_name": "horn",
	"item_name": "clan horn of the hill giants",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

## Get the activator's physical experience object, so we can check if
## they are high enough level for the quest.
eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_PHYSICAL)

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == "home":
	me.SayTo(activator, "\nYes, we still have heavy logistic problems.\nWe don't have half the men we need to control half of the area we should.\nWe don't have enough supply and enough rooms.\nWell, it means all is normal if you do such a heavy invasion like we're doing at the moment.");

elif text[0] == "teach":
	skill = GetSkillNr("polearm mastery")
	sobj = activator.GetSkill(TYPE_SKILL, skill)

	if qm.started() and qm.completed():
		me.SayTo(activator, "\nI can't teach you more.")
	else:
		if eobj != None and eobj.level < 11:
			me.SayTo(activator, "\nYour level is too low. Come back later!")
		else:
			if not qm.finished():
				me.SayTo(activator, "\nFirst bring me the clan horn of the hill giants!")
			else:
				qm.complete()
				activator.Write("Lepash takes %s from your inventory." % quest["item_name"], COLOR_WHITE)

				if sobj != None:
					me.SayTo(activator, "\nYou already know that skill?!")
				else:
					me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Lepash teaches some ancient skill.", COLOR_YELLOW)
					activator.AcquireSkill(skill)

elif text[0] == "polearm" or text[0] == "polearms":
	me.SayTo(activator, "\nPolearm mastery will allow you to fight with polearm weapons. You will do a lot more damage and you will have some protection even though you can't wear a shield using polearms.");

# Accept the quest.
elif text[0] == "accept":
	if not qm.started():
		me.SayTo(activator, "\nReturn to me with the clan horn and I will reward you.")
		qm.start()
	elif qm.completed():
		me.SayTo(activator, "\nThank you for helping us out.")

elif text[0] == "job":
	if not qm.started() or not qm.completed():
		if not qm.finished():
			me.SayTo(activator, "\nThere is somewhere north of Brynknot a hill giant camp.\nPerhaps in a cave or something.\nWe noticed them around here.")

			if eobj != None and eobj.level < 11:
				me.SayTo(activator, "Hmm, your physique level is not high enough.\nCome back after you get stronger and I'll give you more information.", 1)
			else:
				me.SayTo(activator, "You are strong enough.\nFind this hill giant camp and kill the camp leader.\nHe should have a sign of power like a clan horn or something. Show it to me and I will teach you ^polearm^ mastery.", 1)

				if not qm.started():
					me.SayTo(activator, "Do you ^accept^ this quest?", 1)
		else:
			me.SayTo(activator, "\nAh, you are back.\nDo you have the clan horn?\nThen I will ^teach^ you polearm mastery.")
	else:
		me.SayTo(activator, "\nYes, you have done good work with the hill giants.\n")

elif msg == "hello" or msg == "hi" or msg == "hey":
	if not qm.started() or not qm.completed():
		if not qm.finished():
			me.SayTo(activator, "\nHello there. I am guard commander Lepash.\nI have taken ^home^ here in this mercenary guild.\nHmm, are you interested in a ^job^?")
		else:
			if eobj == None or eobj.level < 11:
				me.SayTo(activator, "\nYou have the clan horn?!\nAmazing! When you are higher in level show me the horn again and I will reward you!")
			else:
				me.SayTo(activator, "\nAh, you are back.\nDo you have the clan horn?\nThen I will ^teach^ you polearm mastery.")
	else:
		me.SayTo(activator, "\nHello %s.\nGood to see you back." % activator.name)

else:
	activator.Write("%s listens to you without answer." % me.name, COLOR_WHITE)
