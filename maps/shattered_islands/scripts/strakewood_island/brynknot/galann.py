## @file
## Implements the quest for ring of prayers.

from Atrinik import *
from QuestManager import QuestManager

activator = WhoIsActivator()
me = WhoAmI()

## Info about the quest.
quest = {
	"quest_name": "Galann's Revenge",
	"type": QUEST_TYPE_KILL,
	"kills": 1,
	"message": "Go to the Old Outpost north of Aris in the Giant Mountains, find and kill Torathog the stone giant, and return to Galann in Brynknot.",
}

## Initialize QuestManager.
qm = QuestManager(activator, quest)

msg = WhatIsMessage().strip().lower()
text = msg.split()

def main():
	if msg == "hello" or msg == "hi" or msg == "hey":
		me.SayTo(activator, "\nHello there, {0}.".format(activator.name))

		# Not started?
		if not qm.started():
			me.SayTo(activator, "This is my ^unique^ shop, but I'm afraid I can't identify your items, since I was hurt in this ^fight^ a while ago...", 1)
		elif qm.completed():
			me.SayTo(activator, "Thank you for killing Torathog the stone giant. You are free to use this ^unique^ shop.", 1)
		elif qm.finished():
			me.SayTo(activator, "Amazing! Thank you for killing Torathog the stone giant.\nAs a reward, I'll give you my ring of the prayers. I don't really need it that much these days, and I'm sure you could make better use of it on your adventures.", 1)

			ring = me.CheckInventory(0, "ring_prayers")

			if not ring:
				raise error("Could not find ring inside {0}".format(me.name))

			qm.complete()
			ring.Clone().InsertInside(activator)
			activator.Write("{0} hands you {1}.".format(me.name, ring.GetName()), COLOR_GREEN)
		else:
			me.SayTo(activator, "Have you had any luck killing Torathog the stone giant from the Old Outpost? Or are you here to use my ^unique^ shop?", 1)

	elif msg == "unique":
		me.SayTo(activator, "\nUnique means that everything you sell inside will stay there until the map resets.\nThis is great for selling items you don't need, but others could make use of.")

	elif not qm.started():
		if msg == "fight":
			me.SayTo(activator, "\nA while ago I was hurt badly in a fight with one of the ^stone giants^ from Old Outpost. I can no longer do the job of a smith. If you need a smith, there is one near the Brynknot docks. Failing that, there's superb smiths in Greyton and Asteria.")
		elif msg == "stone giants":
			me.SayTo(activator, "\nWell, I think it was a stone giant from Old Outpost at any rate. I was near the entrance to Old Outpost - north of Aris in the Giant Mountains - taking a walk, when one of them saw me, and hurt me badly in the fight that followed.\nHis name was ^Torathog^, since he kept yelling at me, saying things like 'Torathog will eat ya!'.")
		elif msg == "torathog":
			me.SayTo(activator, "\nI'm still feeling bitter about my loss. Aren't you an adventurer? Would you go find Torathog and kill him for me?\n\n^Sure^\n^No thanks^")
		elif msg == "no thanks":
			me.SayTo(activator, "\nWell suit yourself then. There is, of course, a nice reward involved...\n\n^Okay then^")
		elif msg == "sure" or msg == "okay then":
			me.SayTo(activator, "\nFind and kill Torathog the stone giant in Old Outpost north of Aris in the Giant Mountains, and then return to me with the news of your victory.")
			qm.start()

main()
