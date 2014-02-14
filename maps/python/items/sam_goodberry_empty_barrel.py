from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest
from Packet import Notification

qm = QuestManagerMulti(activator, quest)

def main():
    for (m, x, y) in activator.SquaresAround(1):
        for obj in m.GetLayer(x, y, LAYER_FLOOR):
            if obj.type == Type.FLOOR and obj.name == "clean water":
                me.Remove()
                activator.CreateObject("deserted_island_filled_barrel")
                pl.DrawInfo("\nYou fill the empty barrel up to the brim with the clean water. You should return to Sam Goodberry.", COLOR_YELLOW)
                Notification(activator.Controller(), "Tutorial Available: Weight", "/help basics_weight", "?HELP", 60000)
                qm.start("return barrel")
                qm.complete("fill barrel")
                return

    pl.DrawInfo("\nYou need to stand next to some clean water in order to fill up the empty barrel...", COLOR_YELLOW)

main()
SetReturnValue(1)
