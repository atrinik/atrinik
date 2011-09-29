## @file
## Implements thw wizard Quidton, at the top of the Wizards Tower.

from Interface import Interface
from QuestManager import QuestManager

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

inf = Interface(activator, me)
qm = QuestManager(activator, quest)
qm2 = QuestManager(activator, quest2)

def main():
	if msg == "hello":
		if not qm.started():
			inf.add_msg("Hello there. I am {} the wizard, currently researching the ancient place of magic southeast of this tower.".format(me.name))
			inf.add_link("What is this place of magic you speak of?", dest = "magic")
		elif qm.completed():
			if not qm2.started():
				inf.add_msg("Thank you for your help with the mana crystal.")
				inf.add_msg("Did you know there is a secret to asteroid? It is possible to cast 3 asteroids at once with a single cast, through the use of frost nova.")
				inf.add_link("Tell me more...", dest = "frost nova")
			elif qm2.completed():
				inf.add_msg("Great job fighting those ice giants. I hope frost nova will come in handy.")
			elif qm2.finished():
				inf.add_msg("Good job fighting those ice giants. Now I can teach you frost nova...")
				inf.add_msg_icon("icon_frost_nova.101", "frost nova", True)
				activator.Controller().AcquireSpell(GetSpellNr("frost nova"))
				qm2.complete()
			else:
				inf.add_msg("Don't give up fighting the ice giants just yet! Remember, the portal is right behind me, and I can see you have what it takes to master frost nova.")
		elif qm.finished():
			inf.add_msg("Impressive, you found my mana crystal! Very well then. Let me reward you for your help...")
			inf.add_msg_icon("icon_asteroid.101", "asteroid", True)
			activator.Controller().AcquireSpell(GetSpellNr("asteroid"))
			qm.complete()
		else:
			inf.add_msg("Find my mana crystal the ice golems stole and I will reward you.")
			inf.add_msg("You can access the island the cave is on through the portal I have set up behind me for convenience.")

	# Way to start the first quest.
	elif not qm.started():
		if msg == "magic":
			inf.add_msg("It is an old cave, made of ice. There are many ancient and powerful crystals in that cave that I have been examining, when I noticed that I was missing my mana crystal!")
			inf.add_link("How did that happen?", dest = "mana crystal")

		elif msg == "mana crystal":
			inf.add_msg("I must have dropped it somewhere in the cave, and maybe one of the golems living there picked it up and ran away with it. I have been unable to find it.")
			inf.add_link("Could I help you?", dest = "help")

		elif msg == "help":
			skill = activator.Controller().GetSkill(Type.SKILL, GetSkillNr("wizardry spells"))

			if not skill or skill.level < 45:
				inf.add_msg("Well, the cave is a dangerous place. I don't think you'd be able to survive in there! Come back when you're around level 45 wizardry and it might just work...")
			else:
				inf.add_msg("I could certainly use your help! Very well then. You can access the island the cave is on through the portal I have set up behind me for convenience. Find the golem that stole my mana crystal and I will reward you.")
				qm.start()

	# Only allow this if we completed the first quest.
	elif qm.completed() and not qm2.started():
		if msg == "frost nova":
			inf.add_msg("A very powerful spell, it creates 3 asteroids in front of you at once.")
			skill = activator.Controller().GetSkill(Type.SKILL, GetSkillNr("wizardry spells"))

			if not skill or skill.level < 90:
				inf.add_msg("If only you were level 90 wizardry or higher I could teach you...")
			else:
				inf.add_msg("Hmm! Very well, I can see your determination to learn this spell. I will teach you frost nova, but first you must fight 300 ice giants in the Ice Cave where you helped me find the mana crystal; the portal is just behind me. Fighting them is necessary in order for you to build up strength to cast 3 asteroids at once.")
				qm2.start()

main()
inf.finish()
