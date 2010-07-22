## @file
## Implements quest for the destruction spell, given by Churlgal on
## Strakewood Island.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Ants under Fort Ghzal",
	"type": QUEST_TYPE_KILL,
	"kills": 1,
	"message": "Kill the ant queen under Fort Ghzal and return to Churlgal for a reward in the form of spell of destruction.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

def main():
	# Greeting
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello {0}, I am {1}.".format(activator.name, me.name))

		if not qm.started():
			me.SayTo(activator, "We've had some problems with ants, living under Fort Ghzal.\nIf you kill the ant queen, their leader, I will teach you the knowledge of the spell destruction.\nDo you ^accept^ my quest?", 1)
		elif qm.completed():
			me.SayTo(activator, "Thank you for helping us out.", 1)
		elif qm.finished():
			me.SayTo(activator, "You have done an excellent job!", 1)
			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("wizardry spells"))
			spell = GetSpellNr("destruction")
			spell_level = GetSpell(spell)["level"]

			if not skill:
				me.SayTo(activator, "\nBut you seem to lack the wizardry spells skill...", 1)
			elif skill.level < spell_level:
				me.SayTo(activator, "\nYour wizardry spells skill is too low. Come back later when you train it up more, and I will teach you...", 1)
			else:
				me.SayTo(activator, "\nNow, let me teach you the knowledge of destruction...", 1)
				activator.AcquireSpell(spell, LEARN)
				qm.complete()
		else:
			me.SayTo(activator, "You still need to kill the ant queen. The ant queen can be found by entering the well outside.", 1)

	# Accept the quest.
	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, "\nKill the ant queen under Fort Ghzal and return to me for a reward. The ant queen can be found by entering the well outside.")
			qm.start()
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping us out.")

main()
