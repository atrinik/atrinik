## @file
## Implements generic quest giver to give out different quests for
## different islands.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()
options = GetOptions()

if not options:
	raise StandardError("No event options given. Must be path to the quests.py file for the island.")

# Load the quests file
exec(open(CreatePathname(options)).read())

def main():
	for quest in quest_items:
		## Initialize QuestManager for this quest.
		qm = QuestManager(activator, quest_items[quest]["info"])

		if quest_items[quest]["level"] <= activator.level and (not qm.started() or not qm.completed()):
			break

	## The quest
	quest = quest_items[quest]

	# Check if they are too low level for this quest
	if quest["level"] > activator.level:
		me.SayTo(activator, "\nI have no more quests for you right now.\nCome back when you are stronger.")
		return

	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello {0}, I am {1}, the quest giver.".format(activator.name, me.name))

		if not qm.started():
			me.SayTo(activator, quest["messages"]["not_started"], 1)
			me.SayTo(activator, "Do you ^accept^ this quest?", 1)
		elif qm.completed():
			me.SayTo(activator, "I have no more quests for you.", 1)
		elif qm.finished():
			me.SayTo(activator, "You have done an excellent job! Please take this.", 1)
			# Give out the reward.
			quest_sack = me.FindObject(0, "sack", quest["info"]["quest_name"])

			if not quest_sack or not quest_sack.inv:
				raise StandardError("Quest sack is missing or is empty for quest {0}.".format(quest["info"]["quest_name"]))

			object = quest_sack.inv

			while object:
				object.Clone().InsertInside(activator)
				object = object.below

			activator.Write(quest["messages"]["reward"], COLOR_ORANGE)
			qm.complete()
		else:
			to_kill = qm.num_to_kill()
			me.SayTo(activator, quest["messages"]["not_finished"].format(to_kill, to_kill > 1 and quest["messages"]["kill_suffix_pl"] or quest["messages"]["kill_suffix"]), 1)

	# Accept the quest.
	elif msg == "accept":
		if not qm.started():
			me.SayTo(activator, quest["messages"]["accepted"].format(quest["info"]["kills"]))
			qm.start()

main()
