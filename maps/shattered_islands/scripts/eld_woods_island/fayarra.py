## @file
## Implements Fayarra's Hungry Wolves quest.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Hungry Wolves",
	"type": 1,
	"kills": 7,
	"message": "Kill 7 wolves in a cave East of Clearhaven.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello {0}, I am {1}.".format(activator.name, me.name))

		if not qm.started():
			me.SayTo(activator, "In the past few weeks there have been sightings of wolves near the town, and food disappearing from my storage.\nI think the wolves have been stealing my food. If you helped me and taught the wolves a lesson or two, I would reward you.\nDo you ^accept^ my quest?", 1)
		elif qm.completed():
			me.SayTo(activator, "Thank you for helping me out. The wolves have stopped coming.", 1)
		elif qm.finished():
			me.SayTo(activator, "You have done an excellent job! Please take this.", 1)
			# Give out the reward.
			activator.CreateObjectInside("silvercoin", IDENTIFIED, 2)
			activator.CreateObjectInside("chicken_leg", IDENTIFIED, 5)
			activator.Write("You receive two silver coins and five chicken legs.", COLOR_ORANGE)
			qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "You still need to kill {0} wol{1}.\nThe wolves live in a cave East of here.".format(to_kill, to_kill > 1 and "ves" or "f"), 1)

	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, "\nKill at least {0} wolves in a cave East of here and I will reward you.".format(quest["kills"]))
			qm.start()
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping me out.")

main()
