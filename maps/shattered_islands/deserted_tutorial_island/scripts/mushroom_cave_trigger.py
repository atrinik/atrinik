from Atrinik import *
from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest

activator = WhoIsActivator()
me = WhoAmI()
qm = QuestManagerMulti(activator, quest)

def main():
	SetReturnValue(1)

	if activator.direction != 7:
		return

	activator.Write("\nYou find an abundance of mushrooms...", COLOR_YELLOW)

	if qm.started_part(5) and not qm.completed_part(5) and not qm.finished(5):
		activator.Write("Sam Goodberry will be pleased - you should pick up the mushrooms into the sack he has given you. You reckon seventy mushrooms should be enough...", COLOR_YELLOW)
		activator.Write("Use <b>G</b> (get) key to pick up the selected object below your feet. If you have a container readied or open (ready it using <b>A</b> key, and open it with pressing <b>A</b> again), picked up objects will go into it, instead of going into your inventory. You can drop items from your inventory to the ground using <b>D</b> (drop) key. If you have a container open, you can drop items from your inventory into it, and get items from it into your inventory. This applies to containers on the ground as well, not just the ones in your inventory.", "FDD017")

main()
