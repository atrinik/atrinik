from Atrinik import *
from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest
from Packet import Notification

activator = WhoIsActivator()
me = WhoAmI()
qm = QuestManagerMulti(activator, quest)

def main():
	SetReturnValue(1)

	if me.name == "water barrel":
		return

	for (m, x, y) in activator.SquaresAround(1):
		for obj in m.GetLayer(x, y, LAYER_FLOOR):
			if obj.type == Type.FLOOR and obj.name == "clean water":
				me.name = "water barrel"
				me.face = "barrel_water.101"
				me.weight = 20000
				me.msg = None
				activator.Write("\nYou fill the empty barrel up to the brim with the clean water. You should return to Sam Goodberry.", COLOR_YELLOW)
				Notification(activator.Controller(), "Tutorial Available: Weight", "/help basics_weight", "?HELP", 60000)
				qm.start(4)
				qm.complete(3, sound = False)
				return

	activator.Write("\nYou need to stand next to some clean water in order to fill up the empty barrel...", COLOR_YELLOW)

main()
