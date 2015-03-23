from QuestManager import QuestManager
from InterfaceQuests import escaping_deserted_island

qm = QuestManager(activator, escaping_deserted_island)

def main():
    SetReturnValue(1)

    if qm.finished("get_branches"):
        pl.DrawInfo("You should return with the branches to Sam Goodberry.", COLOR_YELLOW)
        return

    for (m, x, y) in activator.SquaresAround(1):
        for obj in m.GetLayer(x, y, LAYER_WALL):
            if obj.name == "thick branch tree":
                branches = CreateObject("deserted_island_branch")
                branches.nrof = 10
                branches.InsertInto(activator)
                pl.DrawInfo("You cut down ten thick branches from the tree. You should return with the branches to Sam Goodberry.", COLOR_YELLOW)
                return

    pl.DrawInfo("\nStand right next to a tree in order to cut down some branches.", "FDD017")

main()
