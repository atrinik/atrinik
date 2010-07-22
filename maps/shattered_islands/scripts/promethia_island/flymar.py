## @file
## Implements quest for the meteor spell on Promethia Island.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Knowledge of Meteor",
	"type": QUEST_TYPE_KILL,
	"kills": 30,
	"message": "Kill 30 fire demons in the volcano on Promethia Island and Flymar will teach you the spell meteor.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello {0}, I am {1}.".format(activator.name, me.name))

		if not qm.started():
			me.SayTo(activator, "Hmm, do you seek the knowledge of meteor? If you do, I might be able to help...\nDo you ^accept^ my quest?", 1)
		elif qm.completed():
			me.SayTo(activator, "Thank you for helping me out.", 1)
		elif qm.finished():
			me.SayTo(activator, "You have done an excellent job!", 1)
			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("wizardry spells"))
			spell = GetSpellNr("meteor")
			spell_level = GetSpell(spell)["level"]

			if not skill:
				me.SayTo(activator, "\nBut you seem to lack the wizardry spells skill...", 1)
			elif skill.level < spell_level:
				me.SayTo(activator, "\nYour wizardry spells skill is too low. Come back later when you train it up more, and I will teach you...", 1)
			else:
				me.SayTo(activator, "\nNow, let me teach you the knowledge of meteor...", 1)
				activator.AcquireSpell(spell, LEARN)
				qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "You still need to kill {0} fire demon{1}.\nMany of them can be found in the nearby volcano. Use the red portal to enter it.".format(to_kill, to_kill > 1 and "s" or ""), 1)

	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, "\nKill at least {0} fire demons in the nearby volcano and I will teach you the spell meteor. Use the red portal to enter the volcano." .format(quest["kills"]))
			qm.start()
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping me out.")

main()
