## @file
## Script used by the Skillgiver NPC in Developer Testmaps.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()
words = msg.split()

# Learn a skill.
if words[0] == "learn":
	skill = GetSkillNr(msg[6:])

	# Skill was not valid.
	if skill == -1:
		me.SayTo(activator, "\nUnknown skill.")
	else:
		if activator.Controller().DoKnowSkill(skill):
			me.SayTo(activator, "\nYou already know that skill.")
		else:
			activator.Controller().AcquireSkill(skill)

else:
	me.SayTo(activator, "\nI am the Skillgiver.\nSay ^learn <skillname>^ to learn a particular skill.")
