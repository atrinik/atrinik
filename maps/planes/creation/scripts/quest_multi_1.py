## @file
## Example quest showing how QuestManagerMulti works.

from Atrinik import *
from QuestManager import QuestManagerMulti

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

## The quest.
quest = {
	# Name of this quest.
	"quest_name": "Dev Testmaps Multi Quest 1",
	# Type of this quest.
	"type": QUEST_TYPE_MULTI,
	"parts": [
		{
			"type": QUEST_TYPE_KILL,
			"num": 10,
			"message": "Kill 10 bears.",
		},
		{
			"type": QUEST_TYPE_KILL_ITEM,
			"arch_name": "heart",
			"item_name": "Bear's Heart",
			"num": 5,
			"message": "Find 5 bear hearts.",
		},
		{
			"type": QUEST_TYPE_KILL_ITEM,
			"arch_name": "brain",
			"item_name": "Slug's Brain",
			"message": "Find 1 slug brains.",
			"quest_item_keep": 1,
		}
	],
	# Message that will appear in the player's quest list about this quest.
	"message": "Help out this poor NPC.",
}

qm = QuestManagerMulti(activator, quest)

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		if not qm.started():
			me.SayTo(activator, "\nHello {0}. I can demonstrate how multi quest works.\nDo you ^accept^ this quest?".format(activator.name))
		elif qm.completed():
			me.SayTo(activator, "\nThank you for helping me out.")
		elif qm.finished():
			me.SayTo(activator, "\nYou have done an excellent job! As this is just an example quest, there is no reward.")
			qm.complete()
		else:
			me.SayTo(activator, "\nSee quest list for what you have to do.")

	# Accept the quest.
	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, "\nSee quest list.")
			qm.start()

main()
