## @file
## Implements the "Lairwenn's Notes" quest, which allows players to
## acquire a writing pen and the inscription skill.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()
msg = WhatIsMessage().strip().lower()

quest = {
	"quest_name": "Lairwenn's Notes",
	"type": QUEST_TYPE_KILL_ITEM,
	"arch_name": "note",
	"item_name": "Lairwenn's Notes",
	"message": "Lairwenn has lost her notes somewhere in the Brynknot Library and she has asked you to find them for her, as she can't finish the document she is working on without them.",
}

qm = QuestManager(activator, quest)

def main():
	if msg == "hi" or msg == "hey" or msg == "hello":
		if not qm.started():
			me.SayTo(activator, "\nWelcome to the Brynknot library... Please excuse me, I seem to have lost my <a>notes</a> somewhere...")
		elif qm.completed():
			me.SayTo(activator, "\nThank you for finding my notes earlier. Would you like to buy an extra <a=help:writing pen>writing pen</a> for mere {}?\n\n<a>Buy writing pen</a>\n<a>No thanks</a>".format(CostString(me.FindObject(name = "writing pen").value)))
		elif qm.finished():
			me.SayTo(activator, "\nYou found my notes!\n...\nThey were in the luggage on the top floor? How curious... Well, thank you my dear! I think I have something here that may be useful on your adventures... Now, where did I put it...\n<yellow>{} thinks for a moment.</yellow>\nJust pulling your leg my dear! Here it is, a <a=help:writing pen>writing pen</a> will be useful, will it not?".format(me.name))
			activator.Write("You receive a writing pen from the odd librarian.", COLOR_YELLOW)
			me.FindObject(name = "writing pen").Clone().InsertInto(activator)
			activator.Controller().AcquireSkill(GetSkillNr("inscription"))
			qm.complete()
		else:
			me.SayTo(activator, "\n<yellow>{} sighs.</yellow>\nI'm trying to remember where I put them, but I just can't... Please continue your search my dear, they should be somewhere in this library...".format(me.name))

	if not qm.started():
		if msg == "notes":
			me.SayTo(activator, "\nI hurriedly put them away while searching for this book, and then a while later I remembered about the notes but I can't find them now! And I really need them to finish this document I'm working on...\n\n<a>Can I help you?</a>")
		elif msg == "can i help you?":
			me.SayTo(activator, "\nI would certainly appreciate it my dear! The notes are somewhere in this library, I just can't seem to find them...")
			qm.start()
	elif qm.completed():
		if msg == "buy writing pen":
			pen = me.FindObject(name = "writing pen")

			if activator.PayAmount(pen.value):
				activator.Write("You pay {}.".format(CostString(pen.value)), COLOR_WHITE)
				me.SayTo(activator, "\nHere you go dear, a top-quality writing pen!")
				pen.Clone().InsertInto(activator)
			else:
				me.SayTo(activator, "\nOh dear, it seems you don't have enough money!")
		elif msg == "no thanks":
			me.SayTo(activator, "\nMaybe later then!")

main()
