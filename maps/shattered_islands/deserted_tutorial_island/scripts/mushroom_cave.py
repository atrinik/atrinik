from QuestManager import QuestManagerMulti
from Quests import EscapingDesertedIsland as quest

qm = QuestManagerMulti(activator, quest)

def main():
    if qm.started_part(5) and not qm.completed_part(5) and not qm.finished(5):
        me.f_quest_item = True
        me.f_startequip = True
        me.f_identified = True
        return

    SetReturnValue(1)
    pl.DrawInfo("You don't see the need to take so many mushrooms with you...", COLOR_YELLOW)

main()
