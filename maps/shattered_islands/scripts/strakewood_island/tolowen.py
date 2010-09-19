## @file
## Quest for greater healing spell, given by Tolowen, a guard
## in middle of Fort Ghzal.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

quest = {
	"quest_name": "Battle of Fort Ghzal",
	"type": QUEST_TYPE_KILL,
	"kills": 1,
	"message": "Help Fort Ghzal by fighting against orcs north of Fort Ghzal, and then return to Tolowen the guard captain in Fort Ghzal for a reward.",
}

qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello {0}, I am {1}.".format(activator.name, me.name))

		if not qm.started():
			me.SayTo(activator, "We're currently under attack by ^orcs^ north of here. They seem to be coming out of the ^Underground City^.", 1)
		elif qm.completed():
			me.SayTo(activator, "Thank you for your help, we think we can keep the ^orcs^ from the ^Underground City^ at bay now.", 1)
		elif qm.finished():
			me.SayTo(activator, "You have done very well against the ^orcs^.", 1)
			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("divine prayers"))
			spell = GetSpellNr("greater healing")
			spell_level = GetSpell(spell)["level"]

			if not skill:
				me.SayTo(activator, "But you seem to lack the divine prayers skill...", 1)
			elif skill.level < spell_level:
				me.SayTo(activator, "Your divine prayers skill is too low. Come back to me when you're at least level {0} in divine prayers.".format(spell_level), 1)
			else:
				me.SayTo(activator, "Allow me to teach you the knowledge of greater healing...", 1)
				activator.AcquireSpell(spell, LEARN)
				qm.complete()
		else:
			me.SayTo(activator, "Continue fighting the ^orcs^ from the ^Underground City^ a bit more, we almost got them now!", 1)

	elif msg == "underground city":
		me.SayTo(activator, "\nThe Underground City is a huge city under the ground. Even right now, we're standing right above it! It has been populated by ^orcs^ and ^worse creatures^ for a long time now...")

	elif msg == "worse creatures":
		me.SayTo(activator, "\nThere have been rumors of drows, skeletons, ghosts and dragons living in the ^Underground City^.")

	elif msg == "orcs":
		me.SayTo(activator, "\nThe orcs have been around the ^Underground City^ for a long time, but recently have started attacking humans here in Fort Ghzal.")

		if not qm.started():
			me.SayTo(activator, "Do you think you could ^help^ us by fighting the orcs with us?", 1)

	elif msg == "help":
		if not qm.started():
			me.SayTo(activator, "\nHelp us by fighting the ^orcs^ north of here. Then return to me for a reward.")
			qm.start()

main()
