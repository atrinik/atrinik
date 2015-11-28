from Atrinik import *
from QuestManager import QuestManager
from InterfaceQuests import lost_memories


def main():
    found_ant = False
    found_food = False

    for (m, x, y) in activator.SquaresAround(5, AROUND_BLOCKSVIEW, beyond=True):
        for obj in m.Objects(x, y):
            if obj.type == Type.MISC_OBJECT and obj.name in ("straw", "grain"):
                found_food = True
            elif obj.type == Type.MONSTER and obj.race == "ants":
                found_ant = True

        if found_ant and found_food:
            break

    if not found_ant or not found_food:
        return

    SetReturnValue(1)

    pl.DrawInfo("You pour the tonic all around you and instantly the ants "
                "around you seem calmer than before. You should report to "
                "Angela.")
    pl.factions["incuna_barn_ants"] = 2000.0

    qm = QuestManager(activator, lost_memories)
    qm.start(['helping_out', 'ant_trouble', 'the_ants', 'report_angela'])
    qm.complete(['helping_out', 'ant_trouble', 'the_ants', 'befriend'])

    # Destroy the tonic.
    me.Destroy()


main()
