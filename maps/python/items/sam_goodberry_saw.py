from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest

qm = QuestManagerMulti(activator, quest)

def main():
	SetReturnValue(1)

	if qm.finished(6):
		return

	for (m, x, y) in activator.SquaresAround(1):
		for obj in m.GetLayer(x, y, LAYER_WALL):
			if obj.name == "thick branch tree":
				branches = CreateObject("deserted_island_branch")
				branches.nrof = 10
				branches.InsertInto(activator)
				activator.Write("You cut down ten thick branches from the tree. You should return with the branches to Sam Goodberry.", COLOR_YELLOW)
				return

	activator.Write("\nStand right next to a tree in order to cut down some branches.", "FDD017")

main()
