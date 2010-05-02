## @file
## Implements Rirain's Nightmares quest to kill 5 lost souls on Eld Woods
## Island, near the town of Clearhaven.
##
## Reward is one silver coin.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

msg = WhatIsMessage().strip().lower()

## Info about the quest.
quest = {
	"quest_name": "Rirain's Nightmares",
	"type": 1,
	"kills": 5,
	"message": "Kill 5 lost souls near Clearhaven.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

def main():
	# Greeting
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello {0}, I am {1}.".format(activator.name, me.name))

		if not qm.started():
			me.SayTo(activator, "Recently, I have been having terrible nightmares.\nI suspect it's because lately there have been sightings of lost souls in the forest. If you could help me out, that would be great.\nDo you ^accept^ my quest?", 1)
		elif qm.completed():
			me.SayTo(activator, "Thank you for helping me out. I no longer have nightmares.", 1)
		elif qm.finished():
			me.SayTo(activator, "Amazing! You killed the lost souls. Thank you, and please, take this as a token of my gratitude.", 1)
			# Give out the reward.
			activator.CreateObjectInside("silvercoin", IDENTIFIED, 1)
			activator.Write("You receive one silver coin.", COLOR_ORANGE)
			qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "You still need to kill {0} lost soul{1}.\nRemember, lost souls only appear at nighttime.".format(to_kill, to_kill > 1 and "s" or ""), 1)

	# Accept the quest.
	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, "\nKill at least {0} lost souls near the town and I will reward you.\nRemember, lost souls only appear at nighttime.".format(quest["kills"]))
			qm.start()
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping me out.")

main()
