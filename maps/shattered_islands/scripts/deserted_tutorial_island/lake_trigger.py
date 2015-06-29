from Atrinik import *
from QuestManager import QuestManager
from InterfaceQuests import escaping_deserted_island

qm = QuestManager(activator, escaping_deserted_island)

def main():
    SetReturnValue(1)

    if qm.need_complete("explore"):
        pl.DrawInfo("\nYou find a relatively small lake, with seemingly clean water. You try tasting it, and discover that it is quite fresh and delicious. You should report your findings to Sam Goodberry...", COLOR_YELLOW)
        qm.start("explore_report")
        qm.complete("explore")
    elif qm.need_finish("fill_barrel"):
        pl.DrawInfo("\nIn order to fill the empty barrel with water from the lake, stand next to the lake and interact with the water barrel inside your inventory.", COLOR_YELLOW)

main()
