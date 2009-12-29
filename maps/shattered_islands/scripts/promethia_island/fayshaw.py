## @file
## Implements quest for the meteor swarm spell on Promethia Island.

from Atrinik import *
from QuestManager import QuestManager
from threading import Timer

## Activator object.
activator = WhoIsActivator()
## Object who has the event object in their inventory.
me = WhoAmI()

## Get the message.
msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Knowledge of Meteor Swarm",
	"type": QUEST_TYPE_KILL,
	"kills": 300,
	"message": "Kill 300 fire demons in the volcano on Promethia Island and Fayshaw will teach you the spell meteor swarm.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

# Greeting
if msg == "hello" or msg == "hi" or msg == "hey":
	me.SayTo(activator, "\nHello %s, I am %s." % (activator.name, me.name))

	if not qm.started():
		me.SayTo(activator, "Hmm, do you seek the knowledge of meteor swarm? If you do, I might be able to help...\nDo you ^accept^ my quest?", 1)
	elif qm.completed():
		me.SayTo(activator, "Thank you for helping me out.", 1)
	elif qm.finished():
		me.SayTo(activator, "You have done an excellent job!", 1)
		skill = activator.GetSkill(0, GetSkillNr("wizardry spells"))
		spell = GetSpellNr("meteor swarm")
		spell_level = GetSpell(spell)["level"]

		if not skill:
			me.SayTo(activator, "\nBut you seem to lack the wizardry spells skill...", 1)
		elif skill.level < spell_level:
			me.SayTo(activator, "\nYour wizardry spells skill is too low. Come back later when you train it up more, and I will teach you...", 1)
		else:
			me.SayTo(activator, "\nNow, let me teach you the knowledge of meteor swarm...", 1)
			activator.AcquireSpell(spell, LEARN)
			qm.complete()
	else:
		to_kill = qm.num_to_kill()
		me.SayTo(activator, "You still need to kill %d fire demon%s.\nMany of them can be found in the nearby volcano. Use the red portal near my brother Flymar to enter it." % (to_kill, to_kill > 1 and "s" or ""), 1)

# Accept the quest.
elif msg == "accept":
	if not qm.started():
		me.SayTo(activator, "\nKill at least %d fire demons in the nearby volcano and I will teach you the spell meteor swarm. Use the red portal near my brother Flymar to enter the volcano." % quest["kills"])
		qm.start()
	elif qm.completed():
		me.SayTo(activator, "\nThank you for helping me out.")
