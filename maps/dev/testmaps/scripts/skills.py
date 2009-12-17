## @file
## Script used by the Skillgiver NPC in Developer Testmaps.

from Atrinik import *
import string

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
text = msg.split()

# Learn a skill.
if text[0] == "learn":
	## Get the skill number.
	skill = GetSkillNr(text[1])

	# Skill was not valid
	if skill == -1:
		me.SayTo(activator, "Unknown skill.")
	else:
		if activator.DoKnowSkill(skill) == 1:
			me.SayTo(activator, "You already know that skill.")
		else:
			activator.AcquireSkill(skill)

else:
	me.SayTo(activator, "\nI am the Skillgiver.\nSay ^learn <skillname>^ to learn a particular skill.")
