## @file
## Script for the Skills Teacher, who allows plays to learn any skill.

from Atrinik import *

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		me.SayTo(activator, "\nHello. I am {} -- I can teach you any skill if you say <a>learn skill</a>.".format(me.name))

	# Learn a skill.
	elif msg.startswith("learn "):
		skill = GetSkillNr(msg[6:])

		if skill == -1:
			me.SayTo(activator, "\nUnknown skill.")
			return

		if activator.Controller().DoKnowSkill(skill):
			me.SayTo(activator, "\nYou already know that skill.")
			return

		# Learn the skill.
		activator.Controller().AcquireSkill(skill)

main()
