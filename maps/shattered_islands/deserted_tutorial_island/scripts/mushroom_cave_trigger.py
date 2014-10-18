from QuestManager import QuestManager
from InterfaceQuests import escaping_deserted_island
from Packet import Notification

qm = QuestManager(activator, escaping_deserted_island)

def main():
    SetReturnValue(1)

    if activator.direction != 7:
        return

    pl.DrawInfo("\nYou find an abundance of mushrooms...", COLOR_YELLOW)

    if qm.need_finish("get_mushrooms"):
        pl.DrawInfo("Sam Goodberry will be pleased - you should pick up some of the mushrooms and return to him. You reckon seventy mushrooms should be enough...", COLOR_YELLOW)
        Notification(activator.Controller(), "Tutorial Available: Taking and dropping items", "/help basics_taking_dropping", "?HELP", 90000)

main()
