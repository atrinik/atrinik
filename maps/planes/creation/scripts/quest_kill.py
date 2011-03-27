## @file
## Example quest that requires the player to kill X monsters.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## The quest.
quest = {
	# Name of this quest.
	"quest_name": "Dev Testmaps Quest 5",
	# Type of this quest.
	"type": QUEST_TYPE_KILL,
	# How many quest monsters to kill.
	"kills": 5,
	# Message that will appear in the player's quest list about this quest.
	"message": "Kill 5 demiliches.",
}

qm = QuestManager(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nHello {0}. I can demonstrate how 'kill' quest works.\nDo you ^accept^ this quest?".format(activator.name))
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping me out.")
		elif qm.finished():
			me.SayTo(activator, "\nYou have done an excellent job! As this is just an example quest, there is no reward.")
			qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, "\nYou still need to kill {0} demilich{1}.".format(to_kill, to_kill > 1 and "es" or ""))

	# Accept the quest.
	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, "\nKill 10 demiliches on this map.")
			qm.start()

main()
