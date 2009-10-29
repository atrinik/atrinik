## @file
## This script implements quest for two-hand mastery, given by Mercenary
## Taleus in the Brynknot Mercenary Guild.

from Atrinik import *
import string

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Quest item arch name.
quest_arch_name = "tooth"
## Quest item name.
quest_item_name = "elder wyvern tooth"

## Check if the ::activator has a quest object. If so, the quest was
## already completed.
qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
## Check if the ::activator has the quest item we're looking for.
item = activator.CheckInventory(1, quest_arch_name, quest_item_name)
## Get the ::activator's physical experience object, so we can check if
## they are high enough level for the quest.
eobj = activator.GetSkill(TYPE_EXPERIENCE, EXP_PHYSICAL)

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

if text[0] == "archery" or text[0] == "chereth":
	me.SayTo(activator, "\nYou should ask Chereth about the three archery skills.\nShe still teaches archery and her knowledge about it is superior.\nAfter she lost her eyes she was transferred to the Tutorial Island.");

elif text[0] == "teach":
	skill = GetSkillNr("two-hand mastery")
	sobj = activator.GetSkill(TYPE_SKILL, skill)

	if qitem != None:
		me.SayTo(activator, "\nI can't teach you more.")
	else:
		if eobj != None and eobj.level < 10:
			me.SayTo(activator, "\nYou are not strong enough. Come back later!")
		else:
			if item == None:
				me.SayTo(activator, "\nFirst bring me the elder wyvern tooth!")
			else:
				activator.AddQuestObject(quest_arch_name, quest_item_name)
				activator.Write("Taleus takes %s from your inventory." % item.name, COLOR_WHITE)
				item.Remove()

				if sobj != None:
					me.SayTo(activator, "\nYou already know that skill?!")
				else:
					me.map.Message(me.x, me.y, MAP_INFO_NORMAL, "Taleus teaches some ancient skill.", COLOR_YELLOW)
					activator.AcquireSkill(skill)

elif text[0] == "two-hand":
	me.SayTo(activator, "\nTwo-hand mastery will allow you to fight with two-hand weapons. You will do more damage and hit better at the cost of lower protection because you can't wield a shield.");

elif text[0] == "elder" or text[0] == "quest":
	if qitem == None:
		if item == None:
			me.SayTo(activator, "\nThe elder wyverns are the most aggressive and strongest of the wyverns in that cave.");

			if eobj != None and eobj.level >= 10:
				me.SayTo(activator, "Hmm, it seems you are strong enough to try it...\nIf you can kill one or two I will help you too.\nI'll make a deal with you:\nBring me the tooth of an elder wyvern and I will teach you ^two-hand^ mastery.", 1);
			else:
				me.SayTo(activator, "But they are too strong for you at the moment.\nTrain some more melee fighting.\nWhen you have become stronger I will give you a quest.", 1)
		else:
			me.SayTo(activator, "\nAh, you are back.\nDo you have the tooth?\nThen I will ^teach^ you two-hand mastery!")
	else:
		me.SayTo(activator, "\nYes, you have done good work in the wyvern cave.")

elif text[0] == "wyverns" or text[0] == "wyvern":
	if qitem == None:
		if item == None:
			me.SayTo(activator, "\nThe wyverns live in a big cave southeast of Brynknot.\nThey are dangerous and attacked us several times.\nWe have sent some expeditions but there are a lot of them.\nThe biggest problem are the ^elder^ wyverns.")

			if eobj != None and eobj.level <10:
				me.SayTo(activator, "Hmm, your physique level is not high enough.\nCome back after you get stronger and I will have a quest for you!", 1)
			else:
				me.SayTo(activator, "You are strong enough now.\nI have a ^quest^ for you.", 1)
		else:
			me.SayTo(activator, "\nAh, you are back.\nDo you have the tooth?\nThen I will ^teach^ you two-hand mastery!")
	else:
		me.SayTo(activator, "\nYes, you have done good work in the wyvern cave.")

elif msg == "hello" or msg == "hi" or msg == "hey":
	if qitem == None:
		if item == None:
			me.SayTo(activator, "\nHello %s.\nI am the current ^archery^ commander after ^Chereth^ lost her eyes in this terrible fight with the ^wyverns^." % activator.name)
		else:
			if eobj == None or eobj.level < 10:
				me.SayTo(activator, "\nYou have a wyvern tooth?\nAmazing! When you are stronger, bring me the wyvern tooth and I will reward you!")
			else:
				me.SayTo(activator, "\nAh, you are back.\nDo you have the tooth?\nThen I will ^teach^ you two-hand mastery!")
	else:
		me.SayTo(activator, "\nHello %s.\nGood to see you back." % activator.name)

else:
	activator.Write("%s listens to you without answer." % me.name, COLOR_WHITE)
