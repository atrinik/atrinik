## @file
## Quest to recover an old woman's walking stick, which was stolen from her
## in the middle of night at Brynknot Tavern.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

quest = {
	"quest_name": "Melanye's Lost Walking Stick",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "melanye_walking_stick",
	"item_name": "Melanye's Walking Stick",
	"message": "Melanye in Brynknot Tavern has asked you to bring her back her walking stick, which was stolen in the middle of the night by some evil treant, waking up the old woman, who saw the evil treant running to the east...",
}

qm = QuestManager(activator, quest)

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		if not qm.started():
			me.SayTo(activator, "\nWell, hello there my dear. Say -- have you seen any <a>evil treants</a> around the place?")
		elif qm.completed():
			me.SayTo(activator, "\nThank you for recovering my walking stick and teaching that evil treant a lesson!")
		elif qm.finished():
			me.SayTo(activator, "\nOh, you found my walking stick! Thank you, thank you.\n<yellow>You return {}.</yellow>\nHere, it's all I can spare... I hope it will be useful to you.".format(quest["item_name"]))
			reward = CreateObject("silvercoin")
			reward.nrof = 10
			activator.Write("{} hands you {}.".format(me.name, reward.GetName()), COLOR_YELLOW)
			reward.InsertInto(activator)
			qm.complete()
		else:
			me.SayTo(activator, "\nOh... you haven't found my walking stick yet? I'm sure the evil treant ran off to the east of Brynknot...")

	elif not qm.started():
		if msg == "evil treants":
			me.SayTo(activator, "\nI was asleep just last night, when a treant broke into my room here at the tavern, through the window!\n\n<a>How did you know it was a treant?</a>")
		elif msg == "how did you know it was a treant?":
			me.SayTo(activator, "\nWell, it woke me up! I only saw an evil-looking treant running away, holding my enchanted <a>walking stick</a>!")
		elif msg == "walking stick":
			me.SayTo(activator, "\nMy walking stick has been enchanted to give off a soft glow wherever it goes. That is how I saw that it was a treant, running off to the east. I'm not sure what use a walking stick is to a treant, but maybe it just liked the soft glow... But now I have to use an ordinary walking stick... Say, would you look for this treant and recover my walking stick? I might have something in return for your troubles...\n\n<a>Sure</a>")
		elif msg == "sure":
			me.SayTo(activator, "\nWhy, thank you! I'm sure a strong-looking adventurer like yourself will be able to sort this out in no time at all. Just head east of Brynknot -- that is where the treant ran off to...")
			qm.start()

main()
