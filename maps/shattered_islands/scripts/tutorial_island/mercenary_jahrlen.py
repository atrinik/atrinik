## @file
## Script for Jahrlen in Mercenary Guild on Tutorial Island.
##
## Jahrlen gives out probe spell for free, and magic bullet for doing a
## quest. The quest consists of retrieving a rod from goblin in Kobold
## den.

from Atrinik import *
import string, os
from inspect import currentframe

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

execfile(os.path.dirname(currentframe().f_code.co_filename) + "/quests.py")

## Quest item arch name.
quest_arch_name = quest_items["mercenary_jahrlen"]["arch_name"]
## Quest item name.
quest_item_name = quest_items["mercenary_jahrlen"]["item_name"]

## Guild name
guild_tag = "Mercenary"

msg = WhatIsMessage().strip().lower()
text = string.split(msg)

## Check if the activator has a quest object. If so, the quest was
## already completed.
qitem = activator.CheckQuestObject(quest_arch_name, quest_item_name)
## Check if the activator has the quest item we're looking for.
item = activator.CheckInventory(1, quest_arch_name, quest_item_name)

## Function to teach the activator the wizardy skill.
def teach_wizardy():
	## Get the skill number.
	skill = GetSkillNr("wizardry spells")

	if skill == -1:
		me.SayTo(activator, "Unknown skill.")
	else:
		if activator.DoKnowSkill(skill) != 1:
			me.SayTo(activator, "First you need this skill to cast spells.")
			activator.AcquireSkill(skill, LEARN)

if text[0] == "probe":
	guild_force = activator.GetGuildForce()

	if guild_force.slaying != guild_tag:
		me.SayTo(activator, "\nSorry, I can only teach active guild members.")
	else:
		me.SayTo(activator, "\nSo it be! Now you learn the spell probe!")

		# Teach the wizardy skill
		teach_wizardy()

		## Get the spell number.
		spell = GetSpellNr("probe")

		if spell == -1:
			me.SayTo(activator, "\nUnknown spell.")
		else:
			if activator.DoKnowSpell(spell) == 1:
				me.SayTo(activator, "\nYou already know this spell...")
			else:
				activator.AcquireSpell(spell, LEARN)

				me.SayTo(activator, "\nOk, you are ready. Perhaps you want to try the spell?\nYou can safely cast the probe on me to practice.\nJust select the spell and invoke it in my direction.")

elif text[0] == "quest":
	if qitem != None:
		me.SayTo(activator, "\nI have no more quests for you.")
	else:
		if item == None:
			me.SayTo(activator, "\nThe foul kobolds and goblins under the jail have been there long, but recently they started making trouble around here.\nIn the dead of the night, one of the goblins sneaked in here, and stole my old heavy rod of magic bullet!\nIf you can find it, I will teach you the spell magic bullet.\nI bet their chief leader, Bolubal, would have it.\nPlease be careful though. Talk to Frah'ak, the ogre in jail, if you don't know about the kobolds den yet.")
		else:
			me.SayTo(activator, "\nYou found the rod! Incredible! Say ^bullet^ now to learn the spell of magic bullet, as your reward.")

elif text[0] == "bullet":
	guild_force = activator.GetGuildForce()

	if guild_force.slaying != guild_tag:
		me.SayTo(activator, "\nSorry, I can only teach active guild members.")
	else:
		if qitem != None or item == None:
			me.SayTo(activator, "\nI can't teach you this now.")
		else:
			# Teach the wizardy skill
			teach_wizardy()

			spell = GetSpellNr("magic bullet")

			if spell == -1:
				me.SayTo(activator, "\nUnknown spell.")
			else:
				activator.AddQuestObject(quest_arch_name, quest_item_name)
				item.Remove()
				me.SayTo(activator, "\nHere we go!")

				if activator.DoKnowSpell(spell) == 1:
					me.SayTo(activator, "\nYou already know this spell...")
				else:
					activator.AcquireSpell(spell, LEARN)

elif text[0] == "free":
	me.SayTo(activator, "\nI can teach you the spell |probe|.\nProbe is one of the most useful information spells you can learn.\nCast on unknown creatures it will grant you information about their powers and weaknesses.\nThe spell itself is very safe. Creatures will not notice that they were probed. They will not get angry or attack.\nSay ^probe^, then I will teach you the probe spell.\nIf you don't have the needed wizardry skill,\nI will teach it to you too.")

elif text[0] == "chronomancer":
	me.SayTo(activator, "\nYes, I am a master of the chronomancers of Thraal.\nWe are one of the most powerful wizard guilds.\nPerhaps, if you are higher level and stronger...\nHmm... If you ever meet Rangaron in your life... Then tell him that Jahrlen has sent you. Don't ask me more now.")

elif msg == "hello" or msg == "hi" or msg == "hey":
	me.SayTo(activator, "\nHello, I am Jahrlen, War ^Chronomancer^ of Thraal.\nI can teach you the art of magic and your first spells.\nI'll teach you probe for ^free^ and magic bullet for doing a ^quest^.")
