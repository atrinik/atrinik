## @file
## Implements quest for the meteor swarm spell on Promethia Island.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Knowledge of Meteor Swarm",
	"type": QUEST_TYPE_KILL,
	"kills": 300,
	"message": "Kill 300 fire demons in the inner depths of the volcano on Promethia Island - which can be accessed through the entrance near the volcano's middle - and Fayshaw will teach you the spell meteor swarm.",
	"level": 80,
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

def main():
	if not activator.Controller().DoKnowSpell(GetSpellNr("meteor")):
		if msg == "hello" or msg == "hi" or msg == "hey":
			me.SayTo(activator, "\nHello... Sorry... I am busy studying these reports...")

		return

	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nOh... you say my brother, Flymar, sent you? Very well then... My name is {}, as you probably know by now. My brother has been helping me to uncover the secrets of meteor, and we learned about the existence of <a>meteor swarm</a>.".format(me.name))
		elif qm.completed():
			me.SayTo(activator, "\nThanks to you, we both have learned much.")
		elif qm.finished():
			me.SayTo(activator, "\nVery impressive, {}!\n<yellow>You tell {} about your findings.</yellow>\nVery interesting. Thank you for this information. Now, as I promised, thanks to the information you gathered, you are ready to learn meteor swarm...".format(activator.name, me.name))

			if activator.Controller().GetSkill(Type.SKILL, GetSkillNr("wizardry spells")).level < quest["level"]:
				me.SayTo(activator, "... But it would seem your wizardry spells skills level is too low. You need at least level {} wizardry spells.".format(quest["level"]), True)
				return

			activator.Controller().AcquireSpell(GetSpellNr("meteor swarm"))
			qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "\nSorry, I still need more information. Killing {} more fire demon{} in the nearby volcano's inner depths, which you can access through the entrance near its middle, should work.".format(to_kill, "s" if to_kill > 1 else ""))

	elif not qm.started():
		if msg == "meteor swarm":
			me.SayTo(activator, "\nIt seems to be an improved version of the meteor spell - instead of one meteor being summoned, three are summoned at once instead. As you can imagine, such spell could have quite devastating effects. But we don't have enough data about this spell yet...\n\n<a>Can I help?</a>")
		elif msg == "can i help?":
			if activator.Controller().GetSkill(Type.SKILL, GetSkillNr("wizardry spells")).level < quest["level"]:
				me.SayTo(activator, "\nSorry... I just don't think you would be able to do this just yet. You would need at least level {} wizardry spells... Also we still need more time to prepare as well.".format(quest["level"]))
			else:
				me.SayTo(activator, "\nHm... Yes, I think you could do it. I need you to gather more information about the fire demons in the inner depths of the volcano -- say, killing 300 of them should work. If you manage this, you should be ready to learn meteor swarm.")
				qm.start()

main()
