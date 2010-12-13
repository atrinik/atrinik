## @file
## Implements Lynren's quest.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

quest = {
	"quest_name": "Rescuing Lynren",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "book",
	"item_name": "Lynren's book of holy word",
	"message": "Rescue Lynren the paladin, who is imprisoned in Moroch Temple, by bringing her some scrolls of holy word from her home in Asteria among the various temples.",
}

qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nHello there...\n\n^Are you Lynren?^")
		elif qm.finished():
			me.SayTo(activator, "\nSplendid! Alright, let me have that book... This is what you have to do...")
			activator.Controller().AcquireSpell(GetSpellNr("holy word"))
			activator.Write("You use the holy word prayer to free Lynren the paladin.", COLOR_GREEN)
			me.SayTo(activator, "\nThank you, thank you! I hope to meet you again once more... For now, farewell.")
			qm.complete()

			# Activate the spell effect.
			beacon = LocateBeacon("lynren_lever")
			beacon.env.Apply(beacon.env, APPLY_TOGGLE)
		else:
			me.SayTo(activator, "\nHave you got my book of holy word yet? No?... You can find the book at my home in Asteria, among the various temples... Please make haste.")

	elif not qm.started():
		if msg == "are you lynren?":
			me.Communicate("/sigh")
			me.SayTo(activator, "\nYes, that is my name... Lynren the paladin...\n\n^Why are you here?^")

		elif msg == "why are you here?":
			me.SayTo(activator, "\nI have set out on a mission to destroy all evil, and set this evil place as my first target... But I was overpowered and my magics were subdued by the evil magicians here...\n\n^Anything I can do to help?^\n^I see. Farewell.^")

		elif msg == "anything i can do to help?":
			skill = activator.Controller().GetSkill(Type.SKILL, GetSkillNr("divine prayers"))

			if not skill or skill.level < 35:
				me.SayTo(activator, "\nNo... I am afraid you can't... You don't have the power to break the unholy magics that imprisoned me... You would need at least lvl 35 divine prayers skill...")
			else:
				me.SayTo(activator, "\nWell... You could visit my home in Asteria among the various temples, and bring me my book of holy word... The holy word prayer should work to set me free... Would you do this for me?\n^I will^\n^Farewell^")

		elif msg == "i will":
			me.SayTo(activator, "\nExcellent! Please make haste and bring me my book of holy word from my home.")
			qm.start()

		elif msg == "i see. farewell." or msg == "farewell":
			me.Communicate("/me sobs quietly to herself.")

# Completed
if not qm.completed():
	main()
