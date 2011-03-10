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
	"message": "Kill 30 fire demons in the inner depths of the volcano on Promethia Island - which can be accessed through the entrance near the volcano's middle - and Flymar will teach you the spell meteor.",
	"level": 30,
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nHello {}, my name is {}.\nI am currently studying the <a>inactive volcano</a> on this island...".format(activator.name, me.name))
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping me out - your findings have helped my study a lot. If you haven't already, I suggest you go talk to my brother, Fayshaw, west of here.")
		elif qm.finished():
			me.SayTo(activator, "\nYou have done it! Impressive...\n<yellow>You tell {} about your findings.</yellow>\nInteresting... Well, thank you, {}! It seems you are now ready to learn the knowledge of meteor...".format(me.name, activator.name))

			if activator.Controller().GetSkill(Type.SKILL, GetSkillNr("wizardry spells")).level < quest["level"]:
				me.SayTo(activator, "... But it would seem your wizardry spells skills level is too low. You need at least level {} wizardry spells.".format(quest["level"]), True)
				return

			activator.Controller().AcquireSpell(GetSpellNr("meteor"))
			qm.complete()
			me.SayTo(activator, "I suggest you go talk to my brother, Fayshaw, west of here.", True)
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "\n{}, you still need to kill {} fire demon{} in the nearby volcano's inner depths, which you can access through the entrance near its middle. However, please be extremely careful!".format(activator.name, to_kill, "s" if to_kill > 1 else ""))

	elif not qm.started():
		if msg == "inactive volcano":
			me.SayTo(activator, "\nCurrently it is home to various fire demons, and I'm studying their <a>meteor</a> casting ability.")
		elif msg == "meteor":
			me.SayTo(activator, "\nThe meteor spell can be used to summon a meteor-like object that bursts into flames upon impact. It is a quite powerful spell, and I'm trying to learn as much as I can about it.\n\n<a>Can I help?</a>")
		elif msg == "can i help?":
			if activator.Controller().GetSkill(Type.SKILL, GetSkillNr("wizardry spells")).level < quest["level"]:
				me.SayTo(activator, "\nSorry... You would need at least level {} wizardry spells...".format(quest["level"]))
			else:
				me.SayTo(activator, "\nHm... Alright then. I need you to go visit the volcano's inner depths through the entrance near its middle, and kill at least {} fire demons inside, then come back and tell me about your findings. However, please be extremely careful - the fire demons do not like being disturbed. If you succeed, I will share the knowledge of meteor with you.".format(quest["kills"]))
				qm.start()

main()
