## @file
## Implements the Aris Undead Infestation quest, given by Laeda at Aris
## Temple. Reward is Finger of Death and the explanation that it doesn't
## work well on undead.
##
## Player must be lvl 20+ in divine prayers before being able to start the
## quest.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Aris Undead Infestation",
	"type": QUEST_TYPE_KILL,
	"kills": 20,
	"message": "Kill around 20 undead below Aris Temple Graveyard and return to Laeda at Aris Temple for a reward.",
}

qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nHello there. Hmm, do you happen to know anything about the ^undead^?")
		elif qm.completed():
			me.SayTo(activator, "\nThank you, the undead have stopped coming. Or are you here to learn about ^finger of death^?")
		elif qm.finished():
			me.SayTo(activator, "\nYou've done it! Thank you kind adventurer.\nNow, let me teach you the knowledge of ^finger of death^ as your reward...")
			activator.AcquireSpell(GetSpellNr("finger of death"), LEARN)
			qm.complete()
		else:
			me.SayTo(activator, "\nDon't give up just yet, please! You have to help us and banish those undead under the graveyard for good!")

	# Not started yet.
	elif not qm.started():
		if msg == "undead":
			me.SayTo(activator, "\nWe seem to have an undead ^infestation^ going on under this temple's graveyard.")

		elif msg == "infestation":
			me.SayTo(activator, "\nThey have started coming out of that open grave in the graveyard, and no matter how many we kill, they keep coming back! We're just not strong enough to banish them once and for all. That is why the door to the graveyard has been ^sealed off^ by magic.")

		elif msg == "sealed off":
			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("divine prayers"))

			if not skill or skill.level < 20:
				me.SayTo(activator, "\nIf only you were around level 20 in the divine prayers skill, you could help us out...")
			else:
				me.SayTo(activator, "\nYou seem mighty, adventurer, and up for the task! Very well then, go and banish those undead please! I will lift the seal for you.")
				# Create a force that will allow the player to get through
				# the sealed door.
				force = activator.CreatePlayerForce("aris_temple_graveyard")
				force.slaying = "aris_temple_graveyard"
				qm.start()

	# Completed,
	elif qm.completed():
		# Explain about finger of death.
		if msg == "finger of death":
			me.SayTo(activator, "\nA mighty weapon against most opponents. The priest points their finger at the selected enemy and the enemy gets struck by the power of the priest's god! Unfortunately undead are completely immune to it, so it didn't help us in the infestation. In fact it made them stronger!")

main()
