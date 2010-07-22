## @file
## Script for Fenthal the priest in Greyton.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Finger of Death",
	"type": QUEST_TYPE_KILL,
	"kills": 20,
	"message": "Kill 20 red ants Southwest of Greyton and Fenthal from Greyton will teach you the knowledge of Finger of Death.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello {0}, I am {1}.".format(activator.name, me.name))

		if not qm.started():
			me.SayTo(activator, "Recently there have been problems with red ants Southwest of Greyton.\nWe need you to help us deal with them.\nDo you ^accept^ my quest?", 1)
		elif qm.completed():
			me.SayTo(activator, "Thank you for helping us out.", 1)
		elif qm.finished():
			me.SayTo(activator, "You have done an excellent job!", 1)
			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("divine prayers"))
			spell = GetSpellNr("finger of death")
			spell_level = GetSpell(spell)["level"]

			if not skill:
				me.SayTo(activator, "\nBut you seem to lack the divine prayers skill...", 1)
			elif skill.level < spell_level:
				me.SayTo(activator, "\nYour divine prayers skill is too low. Come back later when you train it up more, and I will teach you...", 1)
			else:
				me.SayTo(activator, "\nNow, let me teach you the knowledge of finger of death.", 1)
				activator.AcquireSpell(spell, LEARN)
				qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "You still need to kill {0} red ant{1}.\nThey live Southwest of Greyton, but they only appear at daytime.".format(to_kill, to_kill > 1 and "s" or ""), 1)

	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, "\nKill at least {0} red ants Southwest of Greyton and I will teach you the knowledge of finger of death.\nRemember though, they only appear at daytime.".format(quest["kills"]))
			qm.start()
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping us out.")

main()
