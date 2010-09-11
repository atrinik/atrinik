from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Quidton's Mana Crystal",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "power_crystal",
	"item_name": "Quidton's Mana Crystal",
	"message": "Recover Quidton's mana crystal from ice golems that took it in Ice Cave on Lyondale Archipelago; you can access it from taking a portal next to Quidton in the Wizards' Tower.",
}

## Info about the second quest.
quest2 = {
	"quest_name": "Improved Asteroid",
	"type": QUEST_TYPE_KILL,
	"kills": 300,
	"message": "You have agreed to practice your asteroid spell by fighting ice giants in the Ice Cave on Lyondale Archipelago. When you finish, Quidton will tell you the secret of casting 3 asteroids at once.",
}

qm = QuestManager(activator, quest)
qm2 = QuestManager(activator, quest2)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nHello there. I am {0} the wizard, currently researching the ancient place of ^magic^ southeast of this tower.".format(me.name))
		elif qm.completed():
			if not qm2.started():
				me.SayTo(activator, "\nThank you for your help with the mana crystal.\nDid you know there is a secret to asteroid? It is possible to cast 3 asteroids at once with a single cast, through the use of ^frost nova^.")
			elif qm2.completed():
				me.SayTo(activator, "\nGreat job fighting those ice giants. I hope frost nova will come in handy.")
			elif qm2.finished():
				me.SayTo(activator, "\nGood job fighting those ice giants. Now I can teach you frost nova...")
				activator.AcquireSpell(GetSpellNr("frost nova"), LEARN)
				qm2.complete()
			else:
				me.SayTo(activator, "\nDon't give up fighting the ice giants just yet! Remember, the portal is right behind me, and I can see you have what it takes to master frost nova.")

		elif qm.finished():
			me.SayTo(activator, "\nImpressive, you found my mana crystal! Very well then. Let me reward you for your help...")
			activator.AcquireSpell(GetSpellNr("asteroid"), LEARN)
			qm.complete()
		else:
			me.SayTo(activator, "\nFind my mana crystal the ice golems stole and I will reward you.\nYou can access the island the cave is on through the portal I have set up behind me for convenience.")

	# Way to start the first quest.
	elif not qm.started():
		if msg == "magic":
			me.SayTo(activator, "\nIt is an old cave, made of ice. There are many ancient and powerful crystals in that cave that I have been examining, when I noticed that I was missing my ^mana crystal^!")

		elif msg == "mana crystal":
			me.SayTo(activator, "\nI must have dropped it somewhere in the cave, and maybe one of the golems living there picked it up and ran away with it. I have been unable to find it.\n\n^Could I help you?^")

		elif msg == "could i help you?":
			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("wizardry spells"))

			if not skill or skill.level < 45:
				me.SayTo(activator, "\nWell, the cave is a dangerous place. I don't think you'd be able to survive in there! Come back when you're around level 45 wizardry and it might just work...")
			else:
				me.SayTo(activator, "\nI could certainly use your help! Very well then. You can access the island the cave is on through the portal I have set up behind me for convenience. Find the golem that stole my mana crystal and I will reward you.")
				qm.start()

	# Only allow this if we completed the first quest.
	elif qm.completed() and not qm2.started():
		if msg == "frost nova":
			me.SayTo(activator, "A very powerful spell, it creates 3 asteroids in front of you at once.")
			skill = activator.GetSkill(TYPE_SKILL, GetSkillNr("wizardry spells"))

			if not skill or skill.level < 90:
				me.SayTo(activator, "If only you were level 90 wizardry or higher I could teach you...", 1)
			else:
				me.SayTo(activator, "Hmm! Very well, I can see your determination to learn this spell. I will teach you frost nova, but first you must fight 300 ice giants in the Ice Cave where you helped me find the mana crystal; the portal is just behind me. Fighting them is necessary in order for you to build up strength to cast 3 asteroids at once.", 1)
				qm2.start()

main()
