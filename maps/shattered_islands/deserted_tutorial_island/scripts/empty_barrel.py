from Atrinik import *
from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest

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
				activator.Write("\nYou fill the empty barrel up to the brim with the clean water. You should return to Sam Goodberry.", COLOR_YELLOW)
				activator.Write("If you have too many heavy items in your inventory, your character's speed will go down. You can check your current carrying weight and carry limit by looking at the inventory window (while not holding <b>Shift</b>). The higher your <b>Strength</b> stat, the more items you can carry.", "FDD017")
				qm.start(4)
				qm.complete(3, sound = False)
				return

	activator.Write("\nStand right next to some clean water in order to fill up this barrel.", "FDD017")

main()
