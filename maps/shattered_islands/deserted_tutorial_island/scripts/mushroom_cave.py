from QuestManager import QuestManagerMulti
from InterfaceQuests import escaping_deserted_island

qm = QuestManagerMulti(activator, escaping_deserted_island)

def main():
    if qm.need_finish("get_mushrooms"):
        me.f_quest_item = True
        me.f_startequip = True
        me.f_identified = True
        return

    SetReturnValue(1)
    pl.DrawInfo("You don't see the need to take so many mushrooms with you...", COLOR_YELLOW)

main()
